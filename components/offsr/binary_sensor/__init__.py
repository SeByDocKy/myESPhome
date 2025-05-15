import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv

from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_OCCUPANCY,
)

DEPENDENCIES = ["offsr"]
CONF_THERMOSTAT_CUT = "thermostat_cut"

ICON_HOT_WATER_TANK = "mdi:water-boiler-alert"

from .. import CONF_OFFSR_ID, OFFSRComponent, offsr_ns

OFFSRBinarySensor = offsr_ns.class_("OFFSRBinarySensor", cg.Component)


CONFIG_SCHEMA = {
    cv.GenerateID(): cv.declare_id(OFFSRBinarySensor),
    
    cv.GenerateID(CONF_OFFSR_ID): cv.use_id(OFFSRComponent),
    cv.Optional(CONF_THERMOSTAT_CUT): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY,
        icon = ICON_HOT_WATER_TANK,
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
        
