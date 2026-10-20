import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_FAIKIN_ID, FaikinComponent

DEPENDENCIES = ["faikin"]

CONF_PROTOCOL_VERSION = "protocol_version"
CONF_MODEL_CODE = "model_code"

TEXT_SENSORS = {
    # S21 protocol version found by the boot probe (FY00, or F8 on older units).
    CONF_PROTOCOL_VERSION: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon="mdi:information-outline",
    ),
    # Model code (FC), decoded from reversed ASCII as described by the community documentation.
    CONF_MODEL_CODE: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon="mdi:air-conditioner",
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FAIKIN_ID): cv.use_id(FaikinComponent),
        **{cv.Optional(key): schema for key, schema in TEXT_SENSORS.items()},
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_FAIKIN_ID])
    for key in TEXT_SENSORS:
        if conf := config.get(key):
            var = await text_sensor.new_text_sensor(conf)
            cg.add(getattr(parent, f"set_{key}_text_sensor")(var))
