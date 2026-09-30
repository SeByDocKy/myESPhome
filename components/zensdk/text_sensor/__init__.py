import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_ZENSDK_ID, MAX_PACKS, ZenSdkComponent

DEPENDENCIES = ["zensdk"]

# Conversion ids MUST match the TextConv enum in zensdk.h.
TEXT_RAW, TEXT_STATE, TEXT_VERSION = range(3)

ICON_STATE = "mdi:battery-sync"
ICON_SN = "mdi:identifier"
ICON_FIRMWARE = "mdi:chip"

# Structure matches sensor/__init__.py's dc_channels/ac/battery restructuring: the
# per-pack entries move under `battery.packs:` (0-indexed -- pack0, pack1, ...)
# instead of the former flat pack1_*..pack6_* keys. Device-level entities (global
# state, firmware versions) have no per-pack/AC/DC home and stay flat at the root,
# like sensor/__init__.py's ROOT_SENSORS.
CONF_BATTERY = "battery"
CONF_PACKS = "packs"

# (config key, zenSDK property, conversion, icon, fallback property or None, diagnostic)
ROOT_TEXT_SENSORS = [
    ("state", "packState", TEXT_STATE, ICON_STATE, None, False),
    # Firmware versions. zenSDK only documents `softVersion` (per pack); the device-level properties below are
    # the ones Zendure-HA reads when the report contains them. Not every model reports every one of them.
    ("firmware_version", "masterSoftVersion", TEXT_VERSION, ICON_FIRMWARE, "masterFirmwareVersion", True),
    ("ac_firmware_version", "acFirmwareVersion", TEXT_VERSION, ICON_FIRMWARE, None, True),
    ("dc_firmware_version", "dcFirmwareVersion", TEXT_VERSION, ICON_FIRMWARE, None, True),
    ("bms_firmware_version", "bmsFirmwareVersion", TEXT_VERSION, ICON_FIRMWARE, None, True),
    ("mppt_firmware_version", "mpptFirmwareVersion", TEXT_VERSION, ICON_FIRMWARE, None, True),
]

# `battery.packs:` -- one entry per battery pack, matched by list position to "packData".
# (config key, zenSDK property, conversion, icon, fallback property or None, diagnostic)
PACK_TEXT_FIELDS = [
    ("state", "state", TEXT_STATE, ICON_STATE, None, False),
    ("sn", "sn", TEXT_RAW, ICON_SN, None, False),
    ("firmware_version", "softVersion", TEXT_VERSION, ICON_FIRMWARE, None, True),
]


def _text_schema(icon, diagnostic):
    return text_sensor.text_sensor_schema(
        icon=icon, **({"entity_category": ENTITY_CATEGORY_DIAGNOSTIC} if diagnostic else {})
    )


PACK_TEXT_SCHEMA = cv.Schema(
    {cv.Optional(key): _text_schema(icon, diag) for key, _prop, _conv, icon, _alt, diag in PACK_TEXT_FIELDS}
)


def _pack_entry(value):
    value = cv.Schema({cv.string: PACK_TEXT_SCHEMA})(value)
    if len(value) != 1:
        raise cv.Invalid(
            "Each 'battery.packs' entry must contain exactly one pack name "
            "(e.g. 'pack0: {sn: {name: ...}}')."
        )
    return value


def _validate_packs_list(value):
    value = cv.ensure_list(_pack_entry)(value)
    cv.Length(min=1, max=MAX_PACKS)(value)
    seen = set()
    for entry in value:
        label = next(iter(entry))
        if label in seen:
            raise cv.Invalid(
                f"Pack name '{label}' is used more than once in "
                f"'battery.packs' -- each pack needs a unique name (pack0, pack1, ...)."
            )
        seen.add(label)
    return value


BATTERY_SCHEMA = cv.Schema({cv.Optional(CONF_PACKS): _validate_packs_list})

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZENSDK_ID): cv.use_id(ZenSdkComponent),
        **{
            cv.Optional(key): _text_schema(icon, diagnostic)
            for key, _prop, _conv, icon, _alt, diagnostic in ROOT_TEXT_SENSORS
        },
        cv.Optional(CONF_BATTERY): BATTERY_SCHEMA,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ZENSDK_ID])

    for key, prop, conv, _icon, alt, _diagnostic in ROOT_TEXT_SENSORS:
        if key in config:
            s = await text_sensor.new_text_sensor(config[key])
            if alt is None:
                cg.add(hub.add_text_sensor(prop, conv, -1, s))
            else:
                cg.add(hub.add_text_sensor(prop, conv, -1, s, alt))

    if CONF_BATTERY in config:
        battery = config[CONF_BATTERY]
        # battery.packs: list position (0-based) -> pack index in "packData", same
        # indexing the hub already used internally before this restructuring.
        for i, entry in enumerate(battery.get(CONF_PACKS, [])):
            _label, pack = next(iter(entry.items()))
            for key, prop, conv, _icon, alt, _diagnostic in PACK_TEXT_FIELDS:
                if key in pack:
                    s = await text_sensor.new_text_sensor(pack[key])
                    if alt is None:
                        cg.add(hub.add_text_sensor(prop, conv, i, s))
                    else:
                        cg.add(hub.add_text_sensor(prop, conv, i, s, alt))
