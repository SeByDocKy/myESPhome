import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    STATE_CLASS_MEASUREMENT,
)

DEPENDENCIES = ["offsr"]
CONF_OUTPUT = "output"
CONF_ERROR  = "error"

from .. import CONF_OFFSR_ID, OFFSRComponent, offsr_ns

ErrorSensor = offsr_ns.class_("ErrorSensor", sensor.Sensor , cg.Component)
OutputSensor = offsr_ns.class_("OutputSensor", sensor.Sensor , cg.Component)


CONFIG_SCHEMA = {
    cv.GenerateID(CONF_OFFSR_ID): cv.use_id(OFFSRComponent),
                
    cv.Optional(CONF_ERROR): sensor.sensor_schema(
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
             ),
    cv.Optional(CONF_OUTPUT): sensor.sensor_schema(
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
             ),         
}

async def to_code(config):
    offsr_component = await cg.get_variable(config[CONF_OFFSR_ID])
    if CONF_ERROR in config:
        sens = await sensor.new_sensor(config[CONF_ERROR])
        cg.add(var.set_error(sens))

    if CONF_OUTPUT in config:
        sens = await sensor.new_sensor(config[CONF_OUTPUT])
        cg.add(var.set_output(sens))
        