import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
    DEVICE_CLASS_CURRENT,
    UNIT_AMPERE,
)

DEPENDENCIES = ["bioffsr"]

CONF_OUTPUT = "output"
CONF_ERROR  = "error"
CONF_TARGET = "target"

ICON_EPSILON = "mdi:epsilon"
ICON_PERCENT = "mdi:percent"
ICON_TARGET = "mdi:target"


from .. import CONF_BIOFFSR_ID, BIOFFSRComponent, bioffsr_ns

BIOFFSRSensor = bioffsr_ns.class_("BIOFFSRSensor", cg.Component)


CONFIG_SCHEMA = {
    cv.GenerateID(): cv.declare_id(BIOFFSRSensor),
    
    cv.GenerateID(CONF_BIOFFSR_ID): cv.use_id(BIOFFSRComponent),
                
    cv.Optional(CONF_ERROR): sensor.sensor_schema(
                accuracy_decimals=2,
                icon = ICON_EPSILON,
                state_class=STATE_CLASS_MEASUREMENT,
             ),
    cv.Optional(CONF_OUTPUT): sensor.sensor_schema(
                accuracy_decimals=2,
                icon = ICON_PERCENT,
                state_class=STATE_CLASS_MEASUREMENT,
             ),
    cv.Optional(CONF_TARGET): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                icon=ICON_TARGET,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
             ),         
}

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    offsr_component = await cg.get_variable(config[CONF_BIOFFSR_ID])
    cg.add(var.set_parent(bioffsr_component))

    if CONF_ERROR in config:
        sens = await sensor.new_sensor(config[CONF_ERROR])
        cg.add(var.set_error_sensor(sens))

    if CONF_OUTPUT in config:
        sens = await sensor.new_sensor(config[CONF_OUTPUT])
        cg.add(var.set_output_sensor(sens))
        
    if CONF_TARGET in config:
        sens = await sensor.new_sensor(config[CONF_TARGET])
        cg.add(var.set_target_sensor(sens))