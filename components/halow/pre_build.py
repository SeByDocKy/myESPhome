"""PlatformIO extra script for halow component.

Ensures firmware binary objects (.mbin.o) are available for linking.
Tries multiple strategies: ninja custom targets, copying pre-built .o
files from the SDK, or direct objcopy from .mbin source files.
"""
import glob
import os
import shutil
import subprocess

Import("env")  # noqa: F821


def _read_sdkconfig_value(build_dir, key, default):
    """Read a Kconfig value from the generated sdkconfig file."""
    search_dir = build_dir
    for _ in range(5):
        search_dir = os.path.dirname(search_dir)
        if not search_dir or search_dir == "/":
            break
        try:
            for fname in os.listdir(search_dir):
                if fname.startswith("sdkconfig."):
                    path = os.path.join(search_dir, fname)
                    with open(path) as f:
                        for line in f:
                            if line.startswith(f"{key}="):
                                return line.split("=", 1)[1].strip().strip('"')
        except OSError:
            pass
    return default


def _find_sdk_paths():
    """Find possible MM-IoT-SDK locations."""
    candidates = [
        os.path.expanduser("~/.esphome/mm-iot-esp32"),
        os.path.expanduser("~/esp/mm-iot-esp32"),
        os.path.expanduser("~/esp/mm-iot-esp32-upstream"),
    ]
    return [os.path.join(c, "framework", "mm_shims") for c in candidates if os.path.isdir(c)]


def _find_mbin(name, sdk_paths):
    """Search for a .mbin file in SDK directories."""
    for sdk_path in sdk_paths:
        framework_root = os.path.dirname(sdk_path)  # framework/
        for root, dirs, files in os.walk(framework_root):
            if name in files:
                return os.path.join(root, name)
    return None


def _objcopy_mbin(mbin_path, obj_path, prefix):
    """Convert a .mbin file to a linkable .o using objcopy."""
    objcopy = shutil.which("xtensa-esp-elf-objcopy")
    if not objcopy:
        pio_path = os.path.expanduser(
            "~/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp-elf-objcopy"
        )
        if os.path.exists(pio_path):
            objcopy = pio_path
    if not objcopy:
        return False

    c_ident = mbin_path.replace("/", "_").replace(".", "_").replace("-", "_")
    if c_ident.startswith("_"):
        c_ident = c_ident[1:]

    os.makedirs(os.path.dirname(obj_path), exist_ok=True)
    result = subprocess.run(
        [
            objcopy, "-I", "binary", "-O", "elf32-xtensa-le", "-B", "xtensa",
            mbin_path, obj_path,
            f"--redefine-sym", f"_binary_{c_ident}_start={prefix}_start",
            f"--redefine-sym", f"_binary_{c_ident}_size={prefix}_size",
            f"--redefine-sym", f"_binary_{c_ident}_end={prefix}_end",
            "--rename-section", ".data=.rodata._fw_mbin,contents,alloc,load,readonly,data",
        ],
        capture_output=True,
    )
    return result.returncode == 0


# --- Main ---

build_dir = env.subst("$BUILD_DIR")
mm_shims_dir = os.path.join(build_dir, "esp-idf", "mm_shims")
sdk_paths = _find_sdk_paths()

# Determine BCF filename from sdkconfig
bcf_file = _read_sdkconfig_value(build_dir, "CONFIG_MM_BCF_FILE", None)
if bcf_file:
    bcf_base = os.path.splitext(bcf_file)[0]
else:
    bcf_base = "bcf_mf16858_us"
    for name in ["MF16858_US", "MF08651_US", "MF08551", "MF08251", "MF03120"]:
        if _read_sdkconfig_value(build_dir, f"CONFIG_MM_BCF_{name}", None) == "y":
            bcf_base = f"bcf_{name.lower()}"
            break

# Objects we need to link
objects = [
    ("mm6108.mbin.o", "firmware_binary", "mm6108.mbin"),
    (f"{bcf_base}.mbin.o", "bcf_binary", f"{bcf_base}.mbin"),
]

ninja = shutil.which("ninja")

for obj_name, prefix, mbin_name in objects:
    obj_path = os.path.join(mm_shims_dir, obj_name)

    if os.path.exists(obj_path):
        continue

    generated = False

    # Strategy 1: ninja (works after first successful cmake configure)
    if ninja and os.path.exists(os.path.join(build_dir, "build.ninja")):
        target = f"esp-idf/mm_shims/{obj_name}"
        result = subprocess.run([ninja, target], cwd=build_dir, capture_output=True)
        if result.returncode == 0 and os.path.exists(obj_path):
            print(f"halow: Generated {obj_name} via ninja")
            generated = True

    # Strategy 2: copy pre-built .o from SDK (Seeed SDK has these)
    if not generated:
        for sdk_mm_shims in sdk_paths:
            candidate = os.path.join(sdk_mm_shims, obj_name)
            if os.path.exists(candidate):
                os.makedirs(os.path.dirname(obj_path), exist_ok=True)
                shutil.copy2(candidate, obj_path)
                print(f"halow: Copied {obj_name} from {sdk_mm_shims}")
                generated = True
                break

    # Strategy 3: objcopy from .mbin source file
    if not generated:
        mbin_path = _find_mbin(mbin_name, sdk_paths)
        if mbin_path and _objcopy_mbin(mbin_path, obj_path, prefix):
            print(f"halow: Generated {obj_name} via objcopy from {mbin_path}")
            generated = True

    if not generated:
        print(f"halow: WARNING: Could not generate {obj_name}")

# Always add to LINKFLAGS
fw_obj = os.path.join(mm_shims_dir, "mm6108.mbin.o")
bcf_obj = os.path.join(mm_shims_dir, f"{bcf_base}.mbin.o")
env.Append(LINKFLAGS=[fw_obj, bcf_obj])
print(f"halow: FW={os.path.exists(fw_obj)} BCF={os.path.exists(bcf_obj)} ({bcf_base})")
