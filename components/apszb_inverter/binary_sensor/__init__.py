import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_CONNECTIVITY

from .. import APSInverter, CONF_INVERTER

DEPENDENCIES = ["apszb"]

CONF_ONLINE = "online"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_INVERTER): cv.use_id(APSInverter),
        cv.Optional(CONF_ONLINE): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY, icon="mdi:solar-power"
        ),
    }
)


async def to_code(config):
    inv = await cg.get_variable(config[CONF_INVERTER])
    if CONF_ONLINE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_ONLINE])
        cg.add(inv.set_online_binary_sensor(sens))
