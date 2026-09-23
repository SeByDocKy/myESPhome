import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from .. import HMSWComponent

CONF_HMSW_ID = "hmsw_id"
CONF_REACHABLE = "reachable"

DEPENDENCIES = ["hmsw"]

_REACHABLE_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class="connectivity",
    entity_category="diagnostic",
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HMSW_ID): cv.use_id(HMSWComponent),
        cv.Optional(CONF_REACHABLE): _REACHABLE_SCHEMA,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HMSW_ID])

    if CONF_REACHABLE in config:
        s = await binary_sensor.new_binary_sensor(config[CONF_REACHABLE])
        cg.add(hub.set_reachable_sensor(s))
