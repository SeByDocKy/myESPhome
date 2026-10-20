import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_OPENFIRENET_ID, OpenFireNetComponent, openfirenet_ns

DEPENDENCIES = ["openfirenet"]

TextSensorKey = openfirenet_ns.enum("TextSensorKey")

# key -> (C++ enum name, schema kwargs). Keep in sync with openfirenet.h.
TEXT_SENSORS = {
    "state": ("TEXT_SENSOR_STATE", dict(icon="mdi:fire")),
    "error": ("TEXT_SENSOR_ERROR", dict(icon="mdi:alert-circle-outline")),
    "model": ("TEXT_SENSOR_MODEL", dict(icon="mdi:stove", entity_category=ENTITY_CATEGORY_DIAGNOSTIC)),
    "firmware": ("TEXT_SENSOR_FIRMWARE", dict(icon="mdi:chip", entity_category=ENTITY_CATEGORY_DIAGNOSTIC)),
    "mode": ("TEXT_SENSOR_MODE", dict(icon="mdi:cog-outline")),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENFIRENET_ID): cv.use_id(OpenFireNetComponent),
        **{
            cv.Optional(key): text_sensor.text_sensor_schema(**kwargs)
            for key, (_, kwargs) in TEXT_SENSORS.items()
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_OPENFIRENET_ID])
    for key, (enum_name, _) in TEXT_SENSORS.items():
        if conf := config.get(key):
            sens = await text_sensor.new_text_sensor(conf)
            cg.add(hub.set_text_sensor(getattr(TextSensorKey, enum_name), sens))
