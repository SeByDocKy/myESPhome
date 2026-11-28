#!/usr/bin/env python3
"""Refresh the generated parts of ../README.md: the full entity reference (`entities` markers) and the register
table (`registers` markers).

Usage:  python3 -I gen_readme.py
"""
import importlib.util
from pathlib import Path

HERE = Path(__file__).resolve().parent
README = HERE.parent / "README.md"

spec = importlib.util.spec_from_file_location("registers", HERE.parent / "registers.py")
registers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(registers)


def build():
    # The two models share the same map (checked below): one table
    first, *others = registers.REGISTERS.values()
    assert all(o == first for o in others), "the models differ: write one table per model"
    lines = ["| Entity (YAML key) | Platform | Address | Type | Unit | Scope | Poll |", "|---|---|---|---|---|---|---|"]
    rows = [(s["register"], p, k, s) for p, specs in first["specs"].items() for k, s in specs.items()]
    for reg, platform, key, s in sorted(rows, key=lambda r: r[0]):
        address = str(reg) if s["count"] == 1 else f"{reg}-{reg + s['count'] - 1}"
        scope = "system" if s["system"] else "unit"
        poll = "slow" if s["scan"] == "low" else "fast"
        lines.append(
            f"| `{key}` | {platform} | {address} | {s['vtype']} | {s.get('unit') or ''} | {scope} | {poll} |"
        )
    return "\n".join(lines)


PLATFORMS = ("sensor", "binary_sensor", "text_sensor", "switch", "number", "select", "output")
# Entities of the hub which have no register behind them
EXTRA = {
    "binary_sensor": [("modbus_connection", "the device answers")],
    "switch": [("heartbeat", "keep-alive of the control mode, on by default")],
}


def describe(s):
    parts = [str(s["register"])]
    if s.get("unit"):
        parts.append(s["unit"])
    if "min" in s:
        parts.append(f"{s['min']:g} to {s['max']:g}, step {s['step']:g}")
    if s.get("options"):
        parts.append("options: " + ", ".join(s["options"]))
    if s.get("states"):
        parts.append("values: " + ", ".join(s["states"].values()))
    if s["system"]:
        parts.append("system")
    return ", ".join(parts)


def emit(tree, indent, lines):
    for name, child in tree.items():
        pad = " " * indent
        if "register" in child:
            lines.append(f"{pad}{name}:  # {describe(child)}")
            lines.append(f"{pad}  name: ${{name}}_{'_'.join(p for p in child['path'] if p != 'packs')}")
        else:
            lines.append(f"{pad}{name}:")
            emit(child, indent + 2, lines)


def platform_block(platform):
    lines = [f"{platform}:", "  - platform: ecoflow_stream", "    ecoflow_stream_id: ecoflow_hub"]
    if platform == "output":
        for key, comment in (("charge_power", "0.0 to 1.0 of the maximum charge power"),
                             ("discharge_power", "0.0 to 1.0 of the maximum discharge power")):
            lines += [f"    {key}:  # {comment}", f"      id: ecoflow_{key}"]
        return lines, 2
    specs = next(iter(registers.REGISTERS.values()))["specs"][platform]
    tree = {}
    for s in specs.values():
        node = tree
        for part in s["path"][:-1]:
            node = node.setdefault(part, {})
        node[s["path"][-1]] = s
    emit(tree, 4, lines)
    for key, comment in EXTRA.get(platform, []):
        lines += [f"    {key}:  # {comment}", f"      name: ${{name}}_{key}"]
    return lines, len(specs) + len(EXTRA.get(platform, []))


def build_entities():
    out = []
    for platform in PLATFORMS:
        lines, count = platform_block(platform)
        out += [f"### {platform} ({count})", ""]
        if platform == "output":
            out += ["Needs `charge_sign` in the hub, see [Power control](#power-control).", ""]
        out += ["```yaml", *lines, "```", ""]
    return "\n".join(out).rstrip()


def replace(text, marker, content):
    begin, end = f"<!-- {marker}:begin -->", f"<!-- {marker}:end -->"
    head, rest = text.split(begin, 1)
    _old, tail = rest.split(end, 1)
    return f"{head}{begin}\n{content}\n{end}{tail}"


def main():
    text = replace(README.read_text(), "entities", build_entities())
    README.write_text(replace(text, "registers", build()))
    print("README updated")


if __name__ == "__main__":
    main()
