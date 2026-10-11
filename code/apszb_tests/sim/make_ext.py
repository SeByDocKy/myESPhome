#!/usr/bin/env python3
"""usage: make_ext.py <component dir> <dest>
Copy the apszb component to <dest>/apszb and strip what only makes sense on a real ESP32 (for the host simulation)."""
import re, shutil, sys, pathlib
src, dest = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
if dest.exists():
    shutil.rmtree(dest)
shutil.copytree(src, dest / "apszb", ignore=shutil.ignore_patterns("__pycache__", "README.md"))
p = dest / "apszb" / "__init__.py"
t = p.read_text()
t = t.replace('DEPENDENCIES = ["esp32"]', "DEPENDENCIES = []")
t = re.sub(r"        esp32\.only_on_variant\(.*?\)\(config\)\n", "", t, flags=re.S)
t = re.sub(r"        # The raw 802\.15\.4 driver.*?esp32\.include_builtin_idf_component\(component\)\n", "", t, flags=re.S)
p.write_text(t)
