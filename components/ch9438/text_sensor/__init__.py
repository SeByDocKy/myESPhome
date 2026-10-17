import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import CH9438_PLATFORM_SCHEMA, CONF_CH9438_ID

DEPENDENCIES = ["ch9438"]

CONF_VERSION = "version"

CONFIG_SCHEMA = CH9438_PLATFORM_SCHEMA.extend(
    {
        cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
            icon="mdi:chip",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_CH9438_ID])
    if CONF_VERSION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_VERSION])
        cg.add(hub.set_version_text_sensor(sens))
