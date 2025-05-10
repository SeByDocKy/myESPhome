import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv

from esphome.const import (
    DEVICE_CLASS_OCCUPANCY,
)

DEPENDENCIES = ["offsr"]
CONF_THERMOSTAT_CUT = "thermostat_cut"

from .. import CONF_OFFSR_ID, OFFSRComponent, offsr_ns

OFFSRBinarySensor = offsr_ns.class_("OFFSRBinarySensor", cg.Component)

# ThermostatcutBinarySensor = offsr_ns.class_("ThermostatcutBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = {
    cv.GenerateID(): cv.declare_id(OFFSRBinarySensor),
    
    cv.GenerateID(CONF_OFFSR_ID): cv.use_id(OFFSRComponent),
    cv.Optional(CONF_THERMOSTAT_CUT): binary_sensor.binary_sensor_schema(
        ThermostatcutBinarySensor,
        device_class=DEVICE_CLASS_OCCUPANCY,
    ),
}

async def to_code(config):

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    offsr_component = await cg.get_variable(config[CONF_OFFSR_ID])
    cg.add(var.set_parent(offsr_component))
    if CONF_THERMOSTAT_CUT in config:
        bsens = await binary_sensor.new_binary_sensor(config[CONF_THERMOSTAT_CUT])
        cg.add(var.set_thermostat_cut_binary_sensor(bsens))
    
    # offsr_component = await cg.get_variable(config[CONF_OFFSR_ID])
    # if thermostat_cut_config := config.get(CONF_THERMOSTAT_CUT):
        # bsens = await binary_sensor.new_binary_sensor(thermostat_cut_config)
        # await cg.register_component(bsens, config)
        # cg.add(bsens.set_parent(offsr_component))

        
