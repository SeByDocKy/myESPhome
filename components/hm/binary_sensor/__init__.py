import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from .. import HMComponent

CONF_HM_ID = "hm_id"
CONF_REACHABLE = "reachable"
CONF_PRODUCING = "producing"

DEPENDENCIES = ["hm"]

_REACHABLE_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class="connectivity",
    entity_category="diagnostic",
)
_PRODUCING_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class="power",
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HM_ID): cv.use_id(HMComponent),
        cv.Optional(CONF_REACHABLE): _REACHABLE_SCHEMA,
        cv.Optional(CONF_PRODUCING): _PRODUCING_SCHEMA,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HM_ID])

    if CONF_REACHABLE in config:
        s = await binary_sensor.new_binary_sensor(config[CONF_REACHABLE])
        cg.add(hub.set_reachable_sensor(s))

    if CONF_PRODUCING in config:
        s = await binary_sensor.new_binary_sensor(config[CONF_PRODUCING])
        cg.add(hub.set_producing_sensor(s))
