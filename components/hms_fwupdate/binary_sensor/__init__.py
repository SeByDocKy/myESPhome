import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from .. import HMSFwUpdateComponent

CONF_HMS_FWUPDATE_ID = "hms_fwupdate_id"
CONF_RUNNING = "running"

DEPENDENCIES = ["hms_fwupdate"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HMS_FWUPDATE_ID): cv.use_id(HMSFwUpdateComponent),
        cv.Optional(CONF_RUNNING): binary_sensor.binary_sensor_schema(
            device_class="running",
            icon="mdi:cog-sync",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HMS_FWUPDATE_ID])

    if CONF_RUNNING in config:
        s = await binary_sensor.new_binary_sensor(config[CONF_RUNNING])
        cg.add(hub.set_running_binary_sensor(s))
