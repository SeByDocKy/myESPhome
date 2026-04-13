import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_OCCUPANCY,
)

DEPENDENCIES = ["dualpidpcm"]
CONF_DEADBAND = "deadband"
CONF_SWAP     = "swap"
ICON_DEADBAND = "mdi:emoticon-dead-outline"
ICON_SWAP     = "mdi:swap-vertical"

from .. import CONF_DUALPIDPCM_ID, DUALPIDPCMComponent, dualpidpcm_ns

DUALPIDPCMBinarySensor = dualpidpcm_ns.class_("DUALPIDPCMBinarySensor", cg.Component)


CONFIG_SCHEMA = {
    cv.GenerateID(): cv.declare_id(DUALPIDPCMBinarySensor),
    
    cv.GenerateID(CONF_DUALPIDPCM_ID): cv.use_id(DUALPIDPCMComponent),
    cv.Optional(CONF_DEADBAND): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY,
        icon = ICON_DEADBAND,
    ),
    cv.Optional(CONF_SWAP): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY,
        icon = ICON_SWAP,
    ),
}

async def to_code(config):

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    dualpidpcm_component = await cg.get_variable(config[CONF_DUALPIDPCM_ID])
    cg.add(var.set_parent(dualpidpcm_component))
    
    if CONF_DEADBAND in config:
        bsens = await binary_sensor.new_binary_sensor(config[CONF_DEADBAND])
        cg.add(var.set_deadband_binary_sensor(bsens))
        
    if CONF_SWAP in config:
        bsens = await binary_sensor.new_binary_sensor(config[CONF_SWAP])
        cg.add(var.set_swap_binary_sensor(bsens))    
        
