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

# (config key, zenSDK property, conversion, pack index or -1, icon, fallback property or None, diagnostic)
TEXT_SENSORS = [
    ("state", "packState", TEXT_STATE, -1, ICON_STATE, None, False),
    # Firmware versions. zenSDK only documents `softVersion` (per pack); the device-level properties below are
    # the ones Zendure-HA reads when the report contains them. Not every model reports every one of them.
    ("firmware_version", "masterSoftVersion", TEXT_VERSION, -1, ICON_FIRMWARE, "masterFirmwareVersion", True),
    ("ac_firmware_version", "acFirmwareVersion", TEXT_VERSION, -1, ICON_FIRMWARE, None, True),
    ("dc_firmware_version", "dcFirmwareVersion", TEXT_VERSION, -1, ICON_FIRMWARE, None, True),
    ("bms_firmware_version", "bmsFirmwareVersion", TEXT_VERSION, -1, ICON_FIRMWARE, None, True),
    ("mppt_firmware_version", "mpptFirmwareVersion", TEXT_VERSION, -1, ICON_FIRMWARE, None, True),
]
for _n in range(1, MAX_PACKS + 1):
    TEXT_SENSORS += [
        (f"pack{_n}_state", "state", TEXT_STATE, _n - 1, ICON_STATE, None, False),
        (f"pack{_n}_sn", "sn", TEXT_RAW, _n - 1, ICON_SN, None, False),
        (f"pack{_n}_firmware_version", "softVersion", TEXT_VERSION, _n - 1, ICON_FIRMWARE, None, True),
    ]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZENSDK_ID): cv.use_id(ZenSdkComponent),
        **{
            cv.Optional(key): text_sensor.text_sensor_schema(
                icon=icon, **({"entity_category": ENTITY_CATEGORY_DIAGNOSTIC} if diagnostic else {})
            )
            for key, _prop, _conv, _pack, icon, _alt, diagnostic in TEXT_SENSORS
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ZENSDK_ID])
    for key, prop, conv, pack, _icon, alt, _diagnostic in TEXT_SENSORS:
        if key in config:
            s = await text_sensor.new_text_sensor(config[key])
            if alt is None:
                cg.add(hub.add_text_sensor(prop, conv, pack, s))
            else:
                cg.add(hub.add_text_sensor(prop, conv, pack, s, alt))
