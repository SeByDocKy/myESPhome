import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
    DEVICE_CLASS_CURRENT,
    UNIT_AMPERE,
)

DEPENDENCIES = ["offsr"]
CONF_OUTPUT = "output"
CONF_ERROR  = "error"
CONF_TARGET = "target"

ICON_EPSILON = "mdi:epsilon"
ICON_GAUGE = "mdi:gauge"
ICON_TARGET = "mdi:target"


from .. import CONF_OFFSR_ID, OFFSRComponent, offsr_ns

OFFSRSensor = offsr_ns.class_("OFFSRSensor", cg.Component)


CONFIG_SCHEMA = {
    cv.GenerateID(): cv.declare_id(OFFSRSensor),
    
    cv.GenerateID(CONF_OFFSR_ID): cv.use_id(OFFSRComponent),
                
    cv.Optional(CONF_ERROR): sensor.sensor_schema(
                accuracy_decimals=2,
                icon = ICON_EPSILON,
                state_class=STATE_CLASS_MEASUREMENT,
             ),
    cv.Optional(CONF_OUTPUT): sensor.sensor_schema(
                accuracy_decimals=2,
                icon = ICON_GAUGE,
                state_class=STATE_CLASS_MEASUREMENT,
             ),
    cv.Optional(CONF_TARGET): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                icon=ICON_TARGET,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
             ),         
}

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    offsr_component = await cg.get_variable(config[CONF_OFFSR_ID])
    cg.add(var.set_parent(offsr_component))

    if CONF_ERROR in config:
        sens = await sensor.new_sensor(config[CONF_ERROR])
        cg.add(var.set_error_sensor(sens))

    if CONF_OUTPUT in config:
        sens = await sensor.new_sensor(config[CONF_OUTPUT])
        cg.add(var.set_output_sensor(sens))
        
    if CONF_TARGET in config:
        sens = await sensor.new_sensor(config[CONF_TARGET])
        cg.add(var.set_target_sensor(sens))
