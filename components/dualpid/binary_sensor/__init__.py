import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_OCCUPANCY,
)

DEPENDENCIES = ["dualpid"]
CONF_DEADBAND = "deadband"
ICON_DEADBAND = "mdi:emoticon-dead-outline"

from .. import CONF_DUALPID_ID, DUALPIDComponent, dualpid_ns

DUALPIDBinarySensor = dualpid_ns.class_("DUALPIDBinarySensor", cg.Component)


CONFIG_SCHEMA = {
    cv.GenerateID(): cv.declare_id(DUALPIDBinarySensor),
    
    cv.GenerateID(CONF_DUALPID_ID): cv.use_id(DUALPIDComponent),
    cv.Optional(CONF_DEADBAND): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY,
        icon = ICON_DEADBAND,
    ),
}

async def to_code(config):

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    dualpid_component = await cg.get_variable(config[CONF_DUALPID_ID])
    cg.add(var.set_parent(dualpid_component))
    
    if CONF_DEADBAND in config:
        bsens = await binary_sensor.new_binary_sensor(config[CONF_DEADBAND])
        cg.add(var.set_deadband_binary_sensor(bsens))
               