import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import BNO055Component, CONF_BNO055_ID

DEPENDENCIES = ["bno055"]

CONF_SYSTEM_STATUS = "system_status"
CONF_SYSTEM_ERROR = "system_error"

ICON_SYSTEM_STATUS = "mdi:chip"
ICON_SYSTEM_ERROR = "mdi:alert-circle-outline"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BNO055_ID): cv.use_id(BNO055Component),
        cv.Optional(CONF_SYSTEM_STATUS): text_sensor.text_sensor_schema(
            icon=ICON_SYSTEM_STATUS,
        ),
        cv.Optional(CONF_SYSTEM_ERROR): text_sensor.text_sensor_schema(
            icon=ICON_SYSTEM_ERROR,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_BNO055_ID])

    for key in (CONF_SYSTEM_STATUS, CONF_SYSTEM_ERROR):
        if key not in config:
            continue
        sens = await text_sensor.new_text_sensor(config[key])
        cg.add(getattr(hub, f"set_{key}_text_sensor")(sens))
