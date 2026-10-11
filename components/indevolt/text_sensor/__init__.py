import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import (
    CONF_INDEVOLT_ID,
    MAX_PACKS,
    IndevoltComponent,
    both,
    g2,
    hub_generation,
    apply_poll,
    make_final_validate,
    named_list,
    with_poll,
)

DEPENDENCIES = ["indevolt"]

# Conversion ids MUST match the TextConv enum in indevolt.h.
TEXT_RAW, TEXT_FIRMWARE, TEXT_WORK_MODE, TEXT_STATE, TEXT_ROLE = range(5)
# Identification kinds MUST match the InfoKind enum in indevolt.h.
INFO_TYPE, INFO_SERIAL, INFO_FIRMWARE = range(3)

ICON_STATE = "mdi:battery-sync"
ICON_MODE = "mdi:cog"
ICON_SN = "mdi:identifier"
ICON_FIRMWARE = "mdi:chip"

CONF_BATTERY = "battery"
CONF_PACKS = "packs"


def _text_schema(icon, diagnostic):
    return text_sensor.text_sensor_schema(
        icon=icon, **({"entity_category": ENTITY_CATEGORY_DIAGNOSTIC} if diagnostic else {})
    )


# Identification read once from GET /rpc/Sys.GetConfig ({"device": {"type", "sn", "f_ver"}}).
# (config key, info kind, icon)
INFO_TEXT_SENSORS = [
    ("device_type", INFO_TYPE, "mdi:battery-unknown"),
    ("serial_number", INFO_SERIAL, ICON_SN),
    ("device_firmware", INFO_FIRMWARE, ICON_FIRMWARE),
]

# Device-level text sensors, flat at the root (like sensor/__init__.py's ROOT_SENSORS).
# (config key, points per generation, conversion, icon, diagnostic)
ROOT_TEXT_SENSORS = [
    ("working_mode", both(7101), TEXT_WORK_MODE, ICON_MODE, True),
    ("state", both(6001), TEXT_STATE, ICON_STATE, False),
    ("role", g2(606), TEXT_ROLE, "mdi:server-network", True),
    ("ems_firmware", g2(1118), TEXT_FIRMWARE, ICON_FIRMWARE, True),
    ("bms_mb_firmware", g2(1109), TEXT_FIRMWARE, ICON_FIRMWARE, True),
    ("pcs_firmware", g2(1119), TEXT_FIRMWARE, ICON_FIRMWARE, True),
    ("dcdc_firmware", g2(1120), TEXT_FIRMWARE, ICON_FIRMWARE, True),
]

# `battery:` main board serial number.
BATTERY_TEXT_SENSORS = [
    ("mb_sn", g2(9008), TEXT_RAW, ICON_SN, True),
]

# `battery.packs:` -- one entry per SFA/PFA battery pack, list position (0-based) i -> pack i+1.
# (config key, points per pack, conversion, icon, diagnostic)
PACK_TEXT_FIELDS = [
    ("sn", [9032, 9051, 9070, 9165, 9218], TEXT_RAW, ICON_SN, False),
    ("dcdc_firmware", [1136, 1138, 1140, 1142, 1098], TEXT_FIRMWARE, ICON_FIRMWARE, True),
    ("bms_firmware", [1137, 1139, 1141, 1143, 1099], TEXT_FIRMWARE, ICON_FIRMWARE, True),
]
PACK_TEXT_SCHEMA = cv.Schema(
    {cv.Optional(key): with_poll(_text_schema(icon, diag)) for key, _points, _conv, icon, diag in PACK_TEXT_FIELDS}
)

BATTERY_SCHEMA = cv.Schema(
    {
        **{cv.Optional(key): with_poll(_text_schema(icon, diag)) for key, _points, _conv, icon, diag in BATTERY_TEXT_SENSORS},
        cv.Optional(CONF_PACKS): named_list(PACK_TEXT_SCHEMA, MAX_PACKS, "pack", "pack0: {sn: {name: ...}}"),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_INDEVOLT_ID): cv.use_id(IndevoltComponent),
        **{cv.Optional(key): _text_schema(icon, True) for key, _kind, icon in INFO_TEXT_SENSORS},
        **{
            cv.Optional(key): with_poll(_text_schema(icon, diag))
            for key, _points, _conv, icon, diag in ROOT_TEXT_SENSORS
        },
        cv.Optional(CONF_BATTERY): BATTERY_SCHEMA,
    }
)


def _entries(config):
    """Yield (label, points per generation, entity config, conversion) for every point-based text sensor."""
    for key, points, conv, _icon, _diag in ROOT_TEXT_SENSORS:
        if key in config:
            yield key, points, config[key], conv

    battery = config.get(CONF_BATTERY, {})
    for key, points, conv, _icon, _diag in BATTERY_TEXT_SENSORS:
        if key in battery:
            yield f"{CONF_BATTERY}.{key}", points, battery[key], conv
    for i, entry in enumerate(battery.get(CONF_PACKS, [])):
        label, pack = next(iter(entry.items()))
        for key, point_list, conv, _icon, _diag in PACK_TEXT_FIELDS:
            if key in pack:
                yield f"{CONF_BATTERY}.{CONF_PACKS}.{label}.{key}", g2(point_list[i]), pack[key], conv


FINAL_VALIDATE_SCHEMA = make_final_validate(_entries)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_INDEVOLT_ID])
    gen = hub_generation(config)

    for key, kind, _icon in INFO_TEXT_SENSORS:
        if key in config:
            s = await text_sensor.new_text_sensor(config[key])
            cg.add(hub.add_info_text_sensor(kind, s))

    for _label, points, conf, conv in _entries(config):
        s = await text_sensor.new_text_sensor(conf)
        cg.add(hub.add_text_sensor(points[gen], conv, s))
        apply_poll(hub, points[gen], conf)
