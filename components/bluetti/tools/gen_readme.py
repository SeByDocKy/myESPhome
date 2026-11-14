#!/usr/bin/env python3
"""Refresh the register tables of ../README.md (between the `registers:begin` / `registers:end` markers) from registers.py.

Usage:  python3 -I gen_readme.py
"""
import importlib.util
from pathlib import Path

HERE = Path(__file__).resolve().parent
README = HERE.parent / "README.md"
BEGIN, END = "<!-- registers:begin -->", "<!-- registers:end -->"

spec = importlib.util.spec_from_file_location("registers", HERE.parent / "registers.py")
registers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(registers)

PLATFORM_ORDER = ["sensor", "text_sensor", "binary_sensor", "switch", "number", "select"]
KIND_NOTES = {
    "nibble": "nibble of the register",
    "ref_current": "|raw - reference| x scale, 0 = unknown",
    "string": "ASCII, first character in the low byte",
    "serial": "decimal, low word first",
    "version": "major.minor.patch",
    "states": "enumeration",
    "flags": "E-codes, bit b of register n = bit 16n+b",
    "hex": "bit mask shown in hex",
    "bool": "0 / 1",
    "select": "enumeration",
}


def fmt_register(s):
    first, count = s["register"], s["count"]
    return str(first) if count == 1 else f"{first}-{first + count - 1}"


def fmt_type(s):
    v = s["vtype"]
    return {"RAW": f"{s['count']} x U_WORD"}.get(v, v)


def notes(s, platform):
    out = []
    if s["kind"] == "value" and s.get("scale"):
        out.append(f"x {s['scale']:g}")
    if s.get("offset"):
        out.append(f"{s['offset']:+g}")
    if s["kind"] == "nibble":
        out.append("low nibble" if s.get("shift", 0) == 0 else "high nibble")
    if s["kind"] == "ref_current":
        out.append(f"reference {s['reference']}")
    if s["kind"] in KIND_NOTES and s["kind"] != "nibble" and s["kind"] != "ref_current":
        out.append(KIND_NOTES[s["kind"]])
    if platform == "select":
        out.append(", ".join(f"{v} = {k}" for k, v in sorted(s["options"].items(), key=lambda i: i[1])))
    if platform == "number":
        out.append(f"{s['min']}-{s['max']}")
    return "; ".join(o for o in out if o)


def build():
    lines = []
    for model, data in registers.REGISTERS.items():
        lines.append(f"### {registers.MODEL_NAMES[model]} (`{model}`)")
        lines.append("")
        lines.append(registers.MODEL_NOTES[model].capitalize() + ".")
        lines.append("")
        lines.append(
            "Readable blocks: "
            + ", ".join(f"{a}-{b}" if a != b else str(a) for a, b in data["blocks"])
            + f". Longest read: {data['max_span']} registers."
        )
        lines.append("")
        lines.append("| Entity (YAML key) | Platform | Register | Type | Unit | Poll | Notes |")
        lines.append("|---|---|---|---|---|---|---|")
        rows = []
        for platform in PLATFORM_ORDER:
            for key, s in data["specs"].get(platform, {}).items():
                rows.append((s["register"], platform, key, s))
        for _reg, platform, key, s in sorted(rows, key=lambda r: (r[0], PLATFORM_ORDER.index(r[1]), r[2])):
            poll = "slow" if s.get("scan") == "low" else "fast"
            lines.append(
                f"| `{key}` | {platform} | {fmt_register(s)} | {fmt_type(s)} | {s.get('unit', '')} | {poll} | {notes(s, platform)} |"
            )
        lines.append("")
    return "\n".join(lines)


def main():
    text = README.read_text()
    block = f"{BEGIN}\n{build()}\n{END}"
    if BEGIN in text and END in text:
        head, rest = text.split(BEGIN, 1)
        _old, tail = rest.split(END, 1)
        text = head + block + tail
    else:
        marker = "## Regenerating the register map"
        intro = (
            "## Registers\n\nAll registers are holding registers (read with function code 03, written with 06). "
            "32 / 64 bit values come low word first. \"Poll\" tells which modbus_controller reads the entity "
            "(\"slow\" = `slow_modbus_controller_id`, when there is one). The YAML key is the path of the entity "
            "(`battery.soc` is `battery:` / `soc:`).\n\n"
        )
        assert marker in text
        text = text.replace(marker, intro + block + "\n\n" + marker, 1)
    README.write_text(text)
    print("README updated")


if __name__ == "__main__":
    main()
