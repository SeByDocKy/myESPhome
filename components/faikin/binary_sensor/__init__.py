import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_FAIKIN_ID, FaikinComponent

DEPENDENCIES = ["faikin"]

CONF_CONNECTED = "connected"

BINARY_SENSORS = {
    # True while the unit answers on the S21 port.
    CONF_CONNECTED: binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_CONNECTIVITY,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FAIKIN_ID): cv.use_id(FaikinComponent),
        **{cv.Optional(key): schema for key, schema in BINARY_SENSORS.items()},
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_FAIKIN_ID])
    for key in BINARY_SENSORS:
        if conf := config.get(key):
            var = await binary_sensor.new_binary_sensor(conf)
            cg.add(getattr(parent, f"set_{key}_binary_sensor")(var))
