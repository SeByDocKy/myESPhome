import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID

from .. import ryuw122_ns, RYUW122Component

DEPENDENCIES = ["ryuw122"]

CONF_RYUW122_ID = "ryuw122_id"
CONF_LAST_DATA = "last_data"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RYUW122_ID): cv.use_id(RYUW122Component),
        cv.Optional(CONF_LAST_DATA): text_sensor.text_sensor_schema(
            icon="mdi:message-text",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_RYUW122_ID])

    if CONF_LAST_DATA in config:
        sens = await text_sensor.new_text_sensor(config[CONF_LAST_DATA])
        cg.add(parent.set_last_data_text_sensor(sens))
