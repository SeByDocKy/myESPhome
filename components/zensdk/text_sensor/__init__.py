import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from .. import CONF_ZENSDK_ID, MAX_PACKS, ZenSdkComponent

DEPENDENCIES = ["zensdk"]

# Conversion ids MUST match the TextConv enum in zensdk.h.
TEXT_RAW, TEXT_STATE = range(2)

ICON_STATE = "mdi:battery-sync"
ICON_SN = "mdi:identifier"

# (config key, zenSDK property, conversion, pack index or -1, icon)
TEXT_SENSORS = [
    ("state", "packState", TEXT_STATE, -1, ICON_STATE),
]
for _n in range(1, MAX_PACKS + 1):
    TEXT_SENSORS += [
        (f"pack{_n}_state", "state", TEXT_STATE, _n - 1, ICON_STATE),
        (f"pack{_n}_sn", "sn", TEXT_RAW, _n - 1, ICON_SN),
    ]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ZENSDK_ID): cv.use_id(ZenSdkComponent),
        **{
            cv.Optional(key): text_sensor.text_sensor_schema(icon=icon)
            for key, _prop, _conv, _pack, icon in TEXT_SENSORS
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ZENSDK_ID])
    for key, prop, conv, pack, _icon in TEXT_SENSORS:
        if key in config:
            s = await text_sensor.new_text_sensor(config[key])
            cg.add(hub.add_text_sensor(prop, conv, pack, s))
